// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceConnectionIngest.h"

#include "MetaHumanCaptureSourceLog.h"

#include "Misc/FileHelper.h"
#include "Error/ScopeGuard.h"
#include "Utils/MetaHumanStringUtils.h"

#include "Commands/LiveLinkFaceConnectionCommands.h"

#include "Control/Messages/Constants.h"
#include "TimerManager.h"
#include "Editor.h"

#define ENSURE_AND_RET(InCondition, InReturn, InFormat, ...) if (!ensureMsgf(InCondition, InFormat, ##__VA_ARGS__)) { return InReturn; }
#define ENSURE_AND_RET_VOID(InCondition, InFormat, ...) if (!ensureMsgf(InCondition, InFormat, ##__VA_ARGS__)) { return; }
#define CHECK_AND_PRINT_RET(InCondition, InReturn, InFormat, ...) if (!InCondition) { UE_LOG(LogMetaHumanCaptureSource, Warning, InFormat, ##__VA_ARGS__); return InReturn; }

#define LOCTEXT_NAMESPACE "LiveLinkFaceConnectionIngest"


FLiveLinkFaceConnectionIngest::FLiveLinkFaceConnectionIngest(const FString& InDeviceIPAddress,
                                                             uint16 InDeviceControlPort,
															 bool bInShouldCompressDepthFiles)
    : FLiveLinkFaceIngestBase(bInShouldCompressDepthFiles)
	, DeviceIPAddress { InDeviceIPAddress }
    , DeviceControlPort { InDeviceControlPort }
	, ExportClient{ nullptr }
	, bIsConnected{ false }
	, CommsThread{ TQueueRunner<FCommsRequestParams>::FOnProcess::CreateRaw(this, &FLiveLinkFaceConnectionIngest::ConnectControlClient)}
{
	FControlMessenger::FOnDisconnect ControlClientDisconnected = FControlMessenger::FOnDisconnect::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnControlClientDisconnected);
	ControlMessenger.RegisterDisconnectHandler(MoveTemp(ControlClientDisconnected));
}

FLiveLinkFaceConnectionIngest::~FLiveLinkFaceConnectionIngest()
{
	Shutdown();
}

void FLiveLinkFaceConnectionIngest::Startup(ETakeIngestMode InMode)
{
	FLiveLinkFaceIngestBase::Startup(InMode);

	RegisterCommand(FStartCaptureCommandArgs::CommandName, FCommandHandler::FExecutor::CreateRaw(this, &FLiveLinkFaceConnectionIngest::StartCaptureHandler));
	RegisterCommand(FStopCaptureCommandArgs::CommandName, FCommandHandler::FExecutor::CreateRaw(this, &FLiveLinkFaceConnectionIngest::StopCaptureHandler));

	RegisterForAllEvents();

	PublishEvent<FConnectionChangedEvent>(FConnectionChangedEvent::EState::Disconnected);

	StartConnectTimer();
}

const FString& FLiveLinkFaceConnectionIngest::GetTakesOriginDirectory() const
{
    return TargetIngestBaseDirectory;
}

void FLiveLinkFaceConnectionIngest::Shutdown()
{
    FLiveLinkFaceIngestBase::Shutdown();

	StopConnectTimer();

    ControlMessenger.Stop();

    {
        FScopeLock Lock(&ExportMapMutex);
		ExportMap.Empty();
    }

	ExportClient = nullptr;

	FilesTakeContainsMap.Empty();
}

void FLiveLinkFaceConnectionIngest::GetTakes(const TArray<TakeId>& InTakeIdList, TPerTakeCallback<void> InCallback)
{
	AsyncTask(ENamedThreads::AnyThread, [this, PerTakeCallback = MoveTemp(InCallback), InTakeIdList]() mutable
	{
		bCancelAllRequested = false;

		CurrentTakeIdList.Empty();

		GetTakesCallback = MoveTemp(PerTakeCallback);

		for (const TakeId TakeId : InTakeIdList)
		{
			TakeIngestStopTokens.Add(TakeId, FStopToken());
			TakeProgress[TakeId].store(0.0f);

			{
				FScopeLock lock(&TakeProcessNameMutex);
				TakeProcessName[TakeId] = LOCTEXT("ProgressBarPendingCaption", "Pending...");
			}

			FLiveLinkFaceTakeInfo TakeInfo = GetLiveLinkFaceTakeInfo(TakeId);

			FExportClient::FTakeFileArray ExportArray;
			uint64 TotalLength = 0;
			if (FilesTakeContainsMap.Contains(TakeInfo.TakeMetadata.Identifier))
			{
				const TArray<FGetTakeMetadataResponse::FFileObject>& Files = FilesTakeContainsMap[TakeInfo.TakeMetadata.Identifier];
				for (const FGetTakeMetadataResponse::FFileObject& FileObject : Files)
				{
					ExportArray.Add({ FileObject.Name, FileObject.Length, 0 });
					TotalLength += FileObject.Length;
				}
			}

			TUniquePtr<FFileStream> FileStream = MakeUnique<FFileStream>(TargetIngestBaseDirectory, TakeInfo.TakeMetadata.Identifier, TotalLength);
			FileStream->SetProgressHandler(FFileStream::FReportProgress::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnProgressReport));
			FileStream->SetExportFinished(FFileStream::FExportFinished::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnExportFinished));

			checkf(ExportClient, TEXT("Export client not configured"));

			uint32 TaskId = ExportClient->ExportTakeFiles(TakeInfo.TakeMetadata.Identifier, MoveTemp(ExportArray), MoveTemp(FileStream));
			ExportMap.Add(TaskId, TakeInfo.TakeMetadata.Identifier);
			CurrentTakeIdList.Add(TakeId);
		}
	});
}

void FLiveLinkFaceConnectionIngest::OnProgressReport(const FString& InTakeName, float InProgress)
{
	TProtocolResult<TakeId> FindResult = FindTakeIdByName(InTakeName);
	if (FindResult.IsValid())
	{
		TakeId TakeId = FindResult.ClaimResult();

		TakeProgress[TakeId].store(InProgress);

		FScopeLock lock(&TakeProcessNameMutex);
		FText Transferring = LOCTEXT("ProgressBarDownloadingCaption", "Transferring...");
		if (!Transferring.IdenticalTo(TakeProcessName[TakeId]))
		{
			TakeProcessName[TakeId] = MoveTemp(Transferring);
		}
	}
}

void FLiveLinkFaceConnectionIngest::OnExportFinished(const FString& InTakeName, TProtocolResult<void> InResult)
{
	TProtocolResult<TakeId> FindResult = FindTakeIdByName(InTakeName);
	if (FindResult.IsError())
	{
		return;
	}

	TakeId CurrentTake = FindResult.ClaimResult();

	if (!bIsConnected.load())
	{
		RemoveTakeFromTakeCache(CurrentTake);
		CurrentTakeIdList.Remove(CurrentTake);
		RemoveExportByName(InTakeName);

		TPerTakeResult<void> TakeResult = 
			TPerTakeResult<void>(CurrentTake, FMetaHumanCaptureError(EMetaHumanCaptureError::CommunicationError,
																	 LOCTEXT("IngestError_Connection", "Communication error while transferring data: Control connection lost").ToString()));
		GetTakesCallback(MoveTemp(TakeResult));

		InvokeGetTakesCallbackFromGameThread();

		FScopeLock Lock(&ExportMapMutex);
		if (ExportMap.IsEmpty())
		{
			ClearCachedTakesWithEvent();
		}
	}
	else
	{
		// We are connected and should handle export finished
		if (InResult.IsError())
		{
			FString TakePath = FPaths::Combine(TargetIngestBaseDirectory, InTakeName);
			IFileManager& FileManager = IFileManager::Get();
			FileManager.DeleteDirectory(*TakePath, false, true);

			TPerTakeResult<void> TakeResult = TPerTakeResult<void>(CurrentTake, FMetaHumanCaptureError(EMetaHumanCaptureError::InternalError,
																									   LOCTEXT("IngestError_Communication", "Communication error while transferring data").ToString() + 
																									   TEXT(": ") + 
																									   InResult.GetError().GetMessage()));

			if (IsCancelling() || TakeIngestStopTokens[CurrentTake].IsStopRequested())
			{
				TakeIngestStopTokens.Remove(CurrentTake);

				TakeResult = TPerTakeResult<void>(CurrentTake, FMetaHumanCaptureError(EMetaHumanCaptureError::AbortedByUser));
			}

			GetTakesCallback(MoveTemp(TakeResult));

			CurrentTakeIdList.Remove(CurrentTake);
		}

		RemoveExportByName(InTakeName);

		FScopeLock Lock(&ExportMapMutex);
		if (ExportMap.IsEmpty())
		{
			if (IsCancelling())
			{
				// Cancelling all
				bCancelAllRequested = false;
			}

			// Hacky code
			// The information provided in the CPS take metadata is not enough to fully populate all of the required fields for ingest.
			// As a result, we currently need to read all of the exported take metadata files (take.json, audio, video and depth metadata) in order
			// to populate those values. Some of this is duplicated effort as the protocol JSON objects already contain some of these values.
			// We are effectively treating the take info as an archive source at this point.
			for (const TakeId TakeId : CurrentTakeIdList)
			{
				FScopeLock TakeInfoLock(&TakeInfoCacheMutex);
				FLiveLinkFaceTakeInfo& OldTakeInfo = TakeInfoCache[TakeId];

				const FString TakeDirectory = FPaths::Combine(TargetIngestBaseDirectory, OldTakeInfo.TakeMetadata.Identifier);
				FLiveLinkFaceMetadataParser::ParseTakeInfo(TakeDirectory, OldTakeInfo);
				FLiveLinkFaceMetadataParser::ParseVideoMetadata(TakeDirectory, OldTakeInfo.VideoMetadata);
				FLiveLinkFaceMetadataParser::ParseAudioMetadata(TakeDirectory, OldTakeInfo.AudioMetadata);
				FLiveLinkFaceMetadataParser::ParseDepthMetadata(TakeDirectory, OldTakeInfo.DepthMetadata);
				OldTakeInfo.DepthMetadata.bShouldCompressFiles = bShouldCompressDepthFiles;
			}

			FLiveLinkFaceIngestBase::GetTakes(CurrentTakeIdList, MoveTemp(GetTakesCallback));
		}
	}
}

void FLiveLinkFaceConnectionIngest::RefreshTakeListAsync(TCallback<void> InCallback)
{
	ClearCachedTakesWithEvent();

	FilesTakeContainsMap.Empty();

	CommsThread.Add(FCommsRequestParams{ true, MoveTemp(InCallback) });
}

void FLiveLinkFaceConnectionIngest::OnControlClientDisconnected(const FString& InCause)
{
	bool Expected = true; // Connected
	if (bIsConnected.compare_exchange_strong(Expected, false))
	{
		if (!IsCancelling())
		{
			FScopeLock Lock(&ExportMapMutex);

			if (!ExportMap.IsEmpty())
			{
				bCancelAllRequested = true;
				CancelAllExports();
			}
			else
			{
				ClearCachedTakesWithEvent();
			}
		}

		PublishEvent<FConnectionChangedEvent>(FConnectionChangedEvent::EState::Disconnected);

		StartConnectTimer(true);
	}
}

FLiveLinkFaceTakeMetadata FLiveLinkFaceConnectionIngest::CreateTakeMetadata(const FGetTakeMetadataResponse::FTakeObject& InTake)
{
    FLiveLinkFaceTakeMetadata TakeMetadata;

    TakeMetadata.SlateName = InTake.Slate;
    TakeMetadata.AppVersion = InTake.AppVersion;
	TakeMetadata.DeviceModel = InTake.Model;
    TakeMetadata.Subject = InTake.Subject;
    TakeMetadata.Identifier = InTake.Name;
    FDateTime::ParseIso8601(*InTake.DateTime, TakeMetadata.Date);
    TakeMetadata.TakeNumber = InTake.TakeNumber;
    TakeMetadata.NumFrames = InTake.Video.Frames;

    return TakeMetadata;
}

FLiveLinkFaceVideoMetadata FLiveLinkFaceConnectionIngest::CreateVideoMetadata(const FGetTakeMetadataResponse::FVideoObject& InVideo)
{
    FLiveLinkFaceVideoMetadata VideoMetadata;

    VideoMetadata.FrameRate = InVideo.FrameRate;
    VideoMetadata.Resolution.X = InVideo.Width;
    VideoMetadata.Resolution.Y = InVideo.Height;

    return VideoMetadata;
}

FLiveLinkFaceAudioMetadata FLiveLinkFaceConnectionIngest::CreateAudioMetadata(const FGetTakeMetadataResponse::FAudioObject& InAudio)
{
    FLiveLinkFaceAudioMetadata AudioMetadata;

    AudioMetadata.ChannelsPerFrame = InAudio.Channels;
    AudioMetadata.SampleRate = InAudio.SampleRate;
    AudioMetadata.BitsPerChannel = InAudio.BitsPerChannel;

    return AudioMetadata;
}

bool FLiveLinkFaceConnectionIngest::IsProcessing() const
{
	FScopeLock Lock(&ExportMapMutex);
    if (!ExportMap.IsEmpty() && !IsCancelling())
    {
        return true;
    }
    else
    {
        return FLiveLinkFaceIngestBase::IsProcessing();
    }
}

void FLiveLinkFaceConnectionIngest::CancelProcessing(const TArray<TakeId>& InTakeIdList)
{
	if (InTakeIdList.IsEmpty())
	{
		FScopeLock Lock(&ExportMapMutex);

		if (!ExportMap.IsEmpty())
		{
			bCancelAllRequested = true;
			CancelAllExports();

			InvokeGetTakesCallbackFromGameThread();
		}
	}
	else
	{
		FScopeLock Lock(&ExportMapMutex);

		for (TakeId TakeId : InTakeIdList)
		{
			TakeIngestStopTokens[TakeId].RequestStop();

			const FLiveLinkFaceTakeInfo& TakeInfo = GetLiveLinkFaceTakeInfo(TakeId);

			const uint32* TaskId = ExportMap.FindKey(TakeInfo.TakeMetadata.Identifier);
			if (TaskId)
			{
				checkf(ExportClient, TEXT("Export client not configured"));
				ExportClient->AbortExport(*TaskId);
				ExportMap.Remove(*TaskId);
			}

			CancelCleanup(TakeInfo.TakeMetadata.Identifier);
		}
	}

	if (FLiveLinkFaceIngestBase::IsProcessing())
	{
		FLiveLinkFaceIngestBase::CancelProcessing(InTakeIdList);
	}
}

void FLiveLinkFaceConnectionIngest::CancelAllExports()
{
	checkf(ExportClient, TEXT("Export client not configured"));
	ExportClient->AbortAllExports();

	for (const TPair<uint32, FString>& Export : ExportMap)
	{
		CancelCleanup(Export.Value);
	}

	ExportMap.Empty();
}

void FLiveLinkFaceConnectionIngest::CancelCleanup(const FString& InTakeName)
{
	FString DirectoryPath = FPaths::Combine(TargetIngestBaseDirectory, InTakeName);
	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.DirectoryExists(*DirectoryPath))
	{
		FileManager.DeleteDirectory(*DirectoryPath, true, true);
	}
}

TProtocolResult<TakeId> FLiveLinkFaceConnectionIngest::FindTakeIdByName(const FString& InTakeName) const
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	for (const TPair<TakeId, FLiveLinkFaceTakeInfo>& Elem : TakeInfoCache)
	{
		const FLiveLinkFaceTakeInfo& TakeInfo = Elem.Value;
		if (TakeInfo.TakeMetadata.Identifier == InTakeName)
		{
			return TakeInfo.Id;
		}
	}

	return FCaptureProtocolError(TEXT("Can't find take index with given take name"));
}

void FLiveLinkFaceConnectionIngest::RemoveExportByName(const FString& InTakeName)
{
	FScopeLock Lock(&ExportMapMutex);
	const uint32* TaskId = ExportMap.FindKey(InTakeName);
	if (TaskId)
	{
		ExportMap.Remove(*TaskId);
	}
}

void FLiveLinkFaceConnectionIngest::FetchThumbnails(TArray<TakeId> InTakeIdList)
{
	TArray<FString> TakeNames = GetTakeNamesByIds(InTakeIdList);
	if (TakeNames.IsEmpty())
	{
		return;
	}

	TMap<FString, FExportClient::FTakeFileArray> TakeFileArrayMap;

	FString FileName = TEXT("thumbnail.jpg");

	for (FString& Take : TakeNames)
	{
		check(FilesTakeContainsMap.Contains(Take));

		uint64 Length = 0;
		for (const FGetTakeMetadataResponse::FFileObject& FileObject : FilesTakeContainsMap[Take])
		{
			if (FileObject.Name == FileName)
			{
				Length = FileObject.Length;
			}
		}

		if (Length == 0)
		{
			continue;
		}

		FTakeFile TakeFile;
		TakeFile.FileName = FileName;
		TakeFile.Length = Length;
		TakeFile.Offset = 0;

		TakeFileArrayMap.Add(MoveTemp(Take), { MoveTemp(TakeFile) });
	}

	TUniquePtr<FDataStream> DataStream = MakeUnique<FDataStream>();
	DataStream->SetExportFinished(FDataStream::FFileExportFinished::CreateLambda(
		[this](const FString& InTakeName, TProtocolResult<FDataStream::FData> InData)
	{
		if (InData.IsError())
		{
			UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Failed to fetch thumbnail for take: %s"), *InTakeName);
			return;
		}

		FDataStream::FData Data = InData.ClaimResult();

		FScopeLock Lock(&TakeInfoCacheMutex);
		for (TPair<TakeId, FLiveLinkFaceTakeInfo>& Elem : TakeInfoCache)
		{
			FLiveLinkFaceTakeInfo& TakeInfo = Elem.Value;
			if (InTakeName == TakeInfo.TakeMetadata.Identifier)
			{
				TakeInfo.RawThumbnailData = MoveTemp(Data);
				PublishEvent<FThumbnailChangedEvent>(TakeInfo.Id);
			}
		}
	}));

	checkf(ExportClient, TEXT("Export client not configured"));
	ExportClient->ExportFiles(MoveTemp(TakeFileArrayMap), MoveTemp(DataStream));
}

TArray<FString> FLiveLinkFaceConnectionIngest::GetTakeNamesByIds(TArray<TakeId> InTakeIdList)
{
	TArray<FString> TakeNames;

	FScopeLock Lock(&TakeInfoCacheMutex);
	for (const TakeId TakeId : InTakeIdList)
	{
		TakeNames.Add(TakeInfoCache[TakeId].TakeMetadata.Identifier);
	}

	return TakeNames;
}

void FLiveLinkFaceConnectionIngest::StartConnectTimer(bool bInInvokeDelay)
{
	float InvokeDelay = bInInvokeDelay ? -1.0f : 0.0f;

	// Must be in game thread
	if (IsInGameThread())
	{
		StartConnectTimer_GameThread(InvokeDelay);
	}
	else
	{
		// Should we wait until timer is set
		AsyncTask(ENamedThreads::GameThread, [this, InvokeDelay]
		{
			StartConnectTimer_GameThread(InvokeDelay);
		});
	}
}

void FLiveLinkFaceConnectionIngest::StopConnectTimer()
{
	// Must be in game thread
	if (IsInGameThread())
	{
		StopConnectTimer_GameThread();
	}
	else
	{
		// Should we wait until timer is cleared
		AsyncTask(ENamedThreads::GameThread, [this]
		{
			StopConnectTimer_GameThread();
		});
	}
}

void FLiveLinkFaceConnectionIngest::StartConnectTimer_GameThread(float InInvokeDelay)
{
	// The timer manager WILL BE valid before setting a timer
	if (GEditor && GEditor->IsTimerManagerValid())
	{
		GEditor->GetTimerManager()->SetTimer(ConnectionTimer, FTimerDelegate::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnConnectTimer), ConnectInterval, true, InInvokeDelay);
	}
}

void FLiveLinkFaceConnectionIngest::StopConnectTimer_GameThread()
{
	// The timer manager MIGHT BE invalid before clearing a timer
	if (GEditor && GEditor->IsTimerManagerValid())
	{
		GEditor->GetTimerManager()->ClearTimer(ConnectionTimer);
	}
}

void FLiveLinkFaceConnectionIngest::OnConnectTimer()
{
	if (!bIsConnected.load())
	{
		UE_LOG(LogMetaHumanCaptureSource, Verbose, TEXT("Connecting to the server: %s:%d."), *DeviceIPAddress, DeviceControlPort);
		CommsThread.Add(FCommsRequestParams{});
	}
	else
	{
		StopConnectTimer();
	}
}

#define CHECK_LOG_RETURN(InCondition, InFormat, ...) \
	if (!InCondition) \
	{ \
		UE_LOG(LogMetaHumanCaptureSource, Warning, InFormat, ##__VA_ARGS__); \
		Result = FMetaHumanCaptureError{EMetaHumanCaptureError::CommunicationError}; \
		return; \
	}

void FLiveLinkFaceConnectionIngest::ConnectControlClient(FCommsRequestParams InParams)
{
	// Connect
	TResult<void, FMetaHumanCaptureError> Result = ResultOk;
	auto CallbackInvokeGuard = MakeScopeGuard([&] {
			InParams.ClientCallback(MoveTemp(Result));
		});

	bool bIsConnectedLocal = bIsConnected.load();
	if (!bIsConnectedLocal)
	{
		auto MessengerGuard = MakeScopeGuard([&]
		{
			ControlMessenger.Stop();
		});

		if (!ControlMessenger.Start(DeviceIPAddress, DeviceControlPort).IsValid())
		{
			Result = FMetaHumanCaptureError{ EMetaHumanCaptureError::CommunicationError };
			UE_LOG(LogMetaHumanCaptureSource, Verbose, TEXT("Failed to connect to %s."), *DeviceIPAddress);
			return;
		}
		CHECK_LOG_RETURN(ControlMessenger.StartSession().IsValid(), TEXT("Failed to start session for the Control client. Note: Please ensure you are using compatible versions of LLF and UE"));

		CHECK_LOG_RETURN(ControlMessenger.SendRequest(FSubscribeRequest()).IsValid(), TEXT("Failed to subscribe to events for Control client."));

		TProtocolResult<FGetServerInformationResponse> GetServerInformationResult = ControlMessenger.GetServerInformation();
		CHECK_LOG_RETURN(GetServerInformationResult.IsValid(), TEXT("Failed to fetch the Control server information"));
		FGetServerInformationResponse GetServerInformationResponse = GetServerInformationResult.ClaimResult();

		ExportClient = MakeUnique<FExportClient>(DeviceIPAddress, GetServerInformationResponse.GetExportPort());

		TProtocolResult<FGetStateResponse> GetStateResult = ControlMessenger.SendRequest(FGetStateRequest());
		CHECK_LOG_RETURN(GetStateResult.IsValid(), TEXT("Failed to fetch the current state of the Control server"));

		FGetStateResponse GetStateResponse = GetStateResult.ClaimResult();
		PublishEvent<FRecordingStatusChangedEvent>(GetStateResponse.IsRecording());

		PublishEvent<FConnectionChangedEvent>(FConnectionChangedEvent::EState::Connected);

		bIsConnected.store(true);
		StopConnectTimer();

		MessengerGuard.Dismiss();

		UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Connected to the server: %s:%d."), *DeviceIPAddress, DeviceControlPort);
	}

	if (bIsConnectedLocal && InParams.bForceFetchingTakeList)
	{
		TProtocolResult<FGetTakeListResponse> TakeListRequestResult = ControlMessenger.SendRequest(FGetTakeListRequest());
		CHECK_LOG_RETURN(TakeListRequestResult.IsValid(), TEXT("Failed to fetch the take list from the remote host."));

		FGetTakeListResponse TakeListResponse = TakeListRequestResult.ClaimResult();

		FGetTakeMetadataRequest TakeMetadataRequest(TakeListResponse.GetNames());

		TProtocolResult<FGetTakeMetadataResponse> TakeMetadataRequestResult = ControlMessenger.SendRequest(MoveTemp(TakeMetadataRequest));
		CHECK_LOG_RETURN(TakeMetadataRequestResult.IsValid(), TEXT("Failed to fetch the take metadata from the remote host. Note: Please ensure you are using compatible versions of LLF and UE"));

		FGetTakeMetadataResponse TakeMetadataResponse = TakeMetadataRequestResult.ClaimResult();

		// Populate part of the cache (other stuff not needed at this point)
		const TArray<FGetTakeMetadataResponse::FTakeObject> Takes = TakeMetadataResponse.GetTakes();

		TArray<TakeId> NewTakes = AddTakes(Takes);
		PublishEvent<FNewTakesAddedEvent>(NewTakes);

		FetchThumbnails(MoveTemp(NewTakes));
	}
}

bool FLiveLinkFaceConnectionIngest::CheckIfTakeExists(const FString& InTakeName) const
{
	FScopeLock Lock(&TakeInfoCacheMutex);
	for (const TPair<TakeId, FLiveLinkFaceTakeInfo>& Elem : TakeInfoCache)
	{
		const FLiveLinkFaceTakeInfo& TakeInfo = Elem.Value;
		if (TakeInfo.TakeMetadata.Identifier == InTakeName)
		{
			return true;
		}
	}
	return false;
}

TArray<TakeId> FLiveLinkFaceConnectionIngest::AddTakes(const TArray<FGetTakeMetadataResponse::FTakeObject>& InTakeObjects)
{
	TArray<TakeId> NewTakes;

	for (const FGetTakeMetadataResponse::FTakeObject& Take : InTakeObjects)
	{
		if (CheckIfTakeExists(Take.Name))
		{
			continue;
		}

		FLiveLinkFaceTakeInfo LiveLinkFaceTakeInfo;
		LiveLinkFaceTakeInfo.TakeMetadata = CreateTakeMetadata(Take);
		LiveLinkFaceTakeInfo.VideoMetadata = CreateVideoMetadata(Take.Video);
		LiveLinkFaceTakeInfo.AudioMetadata = CreateAudioMetadata(Take.Audio);

		// If this is not an MHA take we want it to appear in the capture manager even if they cannot be ingested.
		if (!TakeContainsFiles(Take, LiveLinkFaceTakeInfo.TakeMetadata.GetMHAFileNames()))
		{
			const FText Message = LOCTEXT("IngestError_UnsupportedTakeFormat", "Unsupported take format.");
			LiveLinkFaceTakeInfo.Issues.Emplace(Message);
		}

		const FString& Slate = LiveLinkFaceTakeInfo.TakeMetadata.SlateName;
		if (!FCString::IsPureAnsi(*Slate))
		{
			const FText Message = LOCTEXT("IngestError_UnsupportedCharactersInSlateName", "Slate name '{0}' contains unsupported text characters.");
			LiveLinkFaceTakeInfo.Issues.Emplace(FText::Format(Message, FText::FromString(Slate)));
		}

		if (MetaHumanStringContainsWhitespace(Slate))
		{
			const FText Message = LOCTEXT("IngestError_UnsupportedWhiteSpaceCharactersInSlateName", "Slate name '{0}' contains unsupported white space character(s).");
			LiveLinkFaceTakeInfo.Issues.Emplace(FText::Format(Message, FText::FromString(Slate)));
		}
 
		const FString& Subject = LiveLinkFaceTakeInfo.TakeMetadata.Subject;
		if (!FCString::IsPureAnsi(*Subject))
		{
			const FText Message = LOCTEXT("IngestError_UnsupportedCharactersInSubjectName", "Subject name '{0}' contains unsupported text characters.");
			LiveLinkFaceTakeInfo.Issues.Emplace(FText::Format(Message, FText::FromString(Subject)));
		}

		if (MetaHumanStringContainsWhitespace(Subject))
		{
			const FText Message = LOCTEXT("IngestError_UnsupportedWhitespaceCharactersInSubjectName", "Subject name '{0}' contains unsupported white space character(s).");
			LiveLinkFaceTakeInfo.Issues.Emplace(FText::Format(Message, FText::FromString(Subject)));
		}

		LiveLinkFaceTakeInfo.TakeOriginDirectory = FPaths::Combine(TargetIngestBaseDirectory, Take.Name);

		NewTakes.Add(AddTakeInfo(MoveTemp(LiveLinkFaceTakeInfo)));

		FilesTakeContainsMap.Emplace(Take.Name, Take.Files);
	}

	return NewTakes;
}

void FLiveLinkFaceConnectionIngest::InvokeGetTakesCallbackFromGameThread()
{
	if (IsInGameThread())
	{
		OnGetTakesFinishedDelegate.ExecuteIfBound(TArray<FMetaHumanTake>());
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			OnGetTakesFinishedDelegate.ExecuteIfBound(TArray<FMetaHumanTake>());
		});
	}
}

bool FLiveLinkFaceConnectionIngest::StartCaptureHandler(TSharedPtr<FBaseCommandArgs> InCommand)
{
	if (bIsConnected.load())
	{
		const FStartCaptureCommandArgs& StartCapture = static_cast<const FStartCaptureCommandArgs&>(*InCommand);

		FStartRecordingTakeRequest Request(StartCapture.SlateName, 
										   StartCapture.TakeNumber,
										   StartCapture.Subject,
										   StartCapture.Scenario,
										   StartCapture.Tags);

		TProtocolResult<FStartRecordingTakeResponse> Response = ControlMessenger.SendRequest(MoveTemp(Request));

		if (Response.IsError())
		{
			UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to start recording for slate %s, take number "), *StartCapture.SlateName, StartCapture.TakeNumber);
			return false;
		}

		UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Recording started for slate %s, take number %d"), *StartCapture.SlateName, StartCapture.TakeNumber);
		return true;
	}

	return false;
}

bool FLiveLinkFaceConnectionIngest::StopCaptureHandler(TSharedPtr<FBaseCommandArgs> InCommand)
{
	if (bIsConnected.load())
	{
		TProtocolResult<FStopRecordingTakeResponse> Response = ControlMessenger.SendRequest(FStopRecordingTakeRequest());

		if (Response.IsError())
		{
			UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to stop recording."));
			return false;
		}

		const FStopCaptureCommandArgs& StopCapture = static_cast<const FStopCaptureCommandArgs&>(*InCommand);

		if (StopCapture.bShouldFetchTake)
		{
			AddTakeByName(Response.GetResult().GetTakeName());
		}

		UE_LOG(LogMetaHumanCaptureSource, Display, TEXT("Recording stopped, resulting take name: %s"), *Response.GetResult().GetTakeName());

		return true;
	}

	return false;
}

void FLiveLinkFaceConnectionIngest::ClearCachedTakesWithEvent()
{
	int32 PreviousTakeCount = ClearTakeInfoCache();

	if (PreviousTakeCount != 0)
	{
		PublishEvent<FTakeListResetEvent>();
	}
}

void FLiveLinkFaceConnectionIngest::RegisterForAllEvents()
{
	ControlMessenger.RegisterUpdateHandler(UE::CPS::AddressPaths::GTakeAdded, FControlUpdate::FOnUpdateMessage::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnEvent));
	ControlMessenger.RegisterUpdateHandler(UE::CPS::AddressPaths::GTakeRemoved, FControlUpdate::FOnUpdateMessage::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnEvent));
	ControlMessenger.RegisterUpdateHandler(UE::CPS::AddressPaths::GTakeUpdated, FControlUpdate::FOnUpdateMessage::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnEvent));
	ControlMessenger.RegisterUpdateHandler(UE::CPS::AddressPaths::GRecordingStatus, FControlUpdate::FOnUpdateMessage::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnEvent));
	ControlMessenger.RegisterUpdateHandler(UE::CPS::AddressPaths::GDiskCapacity, FControlUpdate::FOnUpdateMessage::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnEvent));
	ControlMessenger.RegisterUpdateHandler(UE::CPS::AddressPaths::GBattery, FControlUpdate::FOnUpdateMessage::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnEvent));
	ControlMessenger.RegisterUpdateHandler(UE::CPS::AddressPaths::GThermalState, FControlUpdate::FOnUpdateMessage::CreateRaw(this, &FLiveLinkFaceConnectionIngest::OnEvent));
}

void FLiveLinkFaceConnectionIngest::OnEvent(TSharedPtr<FControlUpdate> InEvent)
{
	const FString& AddressPath = InEvent->GetAddressPath();
	if (AddressPath == UE::CPS::AddressPaths::GTakeAdded)
	{
		TSharedPtr<FTakeAddedUpdate> TakeAdded = StaticCastSharedPtr<FTakeAddedUpdate>(InEvent);
	}
	else if (AddressPath == UE::CPS::AddressPaths::GTakeRemoved)
	{
		TSharedPtr<FTakeRemovedUpdate> TakeRemoved = StaticCastSharedPtr<FTakeRemovedUpdate>(InEvent);
	}
	else if (AddressPath == UE::CPS::AddressPaths::GRecordingStatus)
	{
		TSharedPtr<FRecordingStatusUpdate> RecordingStatus = StaticCastSharedPtr<FRecordingStatusUpdate>(InEvent);

		PublishEvent<FRecordingStatusChangedEvent>(RecordingStatus->IsRecording());
	}
}

void FLiveLinkFaceConnectionIngest::AddTakeByName(const FString& InTakeName)
{
	FGetTakeMetadataRequest TakeMetadataRequest({ InTakeName });

	ControlMessenger.SendAsyncRequest(MoveTemp(TakeMetadataRequest),
									  FControlMessenger::FOnControlResponse<FGetTakeMetadataRequest>::CreateLambda([this](TProtocolResult<FGetTakeMetadataResponse> InResponse)
	{
		if (InResponse.IsValid())
		{
			FGetTakeMetadataResponse Response = InResponse.ClaimResult();
			TArray<TakeId> NewTakes = AddTakes(Response.GetTakes());
			PublishEvent<FNewTakesAddedEvent>(NewTakes);

			FetchThumbnails(MoveTemp(NewTakes));
		}
	}));
}

void FLiveLinkFaceConnectionIngest::RemoveTakeByName(const FString& InTakeName)
{
	TProtocolResult<TakeId> FindResult = FindTakeIdByName(InTakeName);
	if (FindResult.IsError())
	{
		// Do nothing
		return;
	}

	TakeId Id = FindResult.ClaimResult();
	CancelProcessing({ Id });

	RemoveTakeFromTakeCache(Id);

	PublishEvent<FTakesRemovedEvent>(Id);
}

bool FLiveLinkFaceConnectionIngest::TakeContainsFiles(const FGetTakeMetadataResponse::FTakeObject& Take, const TArray<FString>& FileNames)
{
	for (const FString& FileName : FileNames)
	{
		if (!Take.Files.ContainsByPredicate([&FileName](const FGetTakeMetadataResponse::FFileObject& InFile) { return InFile.Name == FileName; }))
		{
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
