// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseIngestLiveLinkDevice.h"

#include "HAL/FileManager.h"
#include "CaptureManagerUnrealEndpointModule.h"
#include "Settings/CaptureManagerSettings.h"

#include "Utils/IngestLiveLinkDeviceUtils.h"
#include "Utils/IngestLiveLinkDeviceLog.h"

#include "CaptureDataConverter.h"

#include "Async/TaskWaiter.h"

DEFINE_LOG_CATEGORY(LogIngestLiveLinkDevice);

namespace UE::CaptureManager::Private
{
static void TryCleanUpWorkingDirectory(const FString& InWorkingDirectory)
{
	const UCaptureManagerSettings* CaptureManagerSettings = GetDefault<UCaptureManagerSettings>();
	check(CaptureManagerSettings);

	const bool bShouldCleanWorkingDirectory = CaptureManagerSettings->bShouldCleanWorkingDirectory;
	if (bShouldCleanWorkingDirectory)
	{
		IFileManager::Get().DeleteDirectory(*InWorkingDirectory, false, true);
		UE_LOG(LogIngestLiveLinkDevice, Display, TEXT("Deleting directory: %s"), *InWorkingDirectory);
	}
}

} // namespace UE::CaptureManager::Private

class UBaseIngestLiveLinkDevice::FImpl : public TSharedFromThis<UBaseIngestLiveLinkDevice::FImpl>
{
public:

	explicit FImpl(UBaseIngestLiveLinkDevice* InOwner);

	void IngestTake(const UIngestCapability_ProcessHandle* InProcessHandle, 
					const UIngestCapability_Options* InIngestOptions, 
					TSharedPtr<UE::CaptureManager::FTaskProgress> InTaskProgress);
	void CancelIngest(int32 InTakeId);
	void OnDeviceRemoved(TArray<int32> TakeIdentifiers);

private:

	FCaptureDataConverterResult<void> RunConversion(int32 InTakeId,
													TStrongObjectPtr<const UIngestCapability_Options> InIngestOptions,
													TSharedPtr<UE::CaptureManager::FTaskProgress> InTaskProgress,
													FCaptureDataConverterParams& InParams);

	struct FTakeUploadInfo
	{
		FGuid TaskId;
		FGuid EndpointID;
	};

	FCriticalSection Mutex;

	// We hold a strong ptr to the parent UBaseIngestLiveLinkDevice, because this impl object could otherwise outlive the parent (it is shared with callbacks)
	TStrongObjectPtr<UBaseIngestLiveLinkDevice> Owner;
	TMap<int32, TSharedRef<FCaptureDataConverter>> Converters;
	TMap<int32, FTakeUploadInfo> TakeUploads;

	// Every invocation of `IngestTake` function will increase a counter in TaskWaiter, registering that task is running.
	// Once function finishes it's work, counter is decremented.
	// And if it happens that device is being removed before counter is equal to zero, TaskWaiter will busy wait for it to reach zero.
	// Ensures that 'IngestTake' does not access potentially destroyed device.
	UE::CaptureManager::FTaskWaiter TaskWaiter;
};

UBaseIngestLiveLinkDevice::FImpl::FImpl(UBaseIngestLiveLinkDevice* InOwner) :
	Owner(InOwner)
{
}

void UBaseIngestLiveLinkDevice::FImpl::OnDeviceRemoved(TArray<int32> TakeIdentifiers)
{	
	for (const int32 TakeIdentifier : TakeIdentifiers)
	{
		CancelIngest(TakeIdentifier);
	}

	while (true)
	{
		FScopeLock TakeUploadsRemoveLock(&Mutex);
		int32 TakesInProgressCount = TakeUploads.Num();
		TakeUploadsRemoveLock.Unlock();

		if (TakesInProgressCount == 0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.1f);
	}

	TaskWaiter.WaitForAll();
}

void UBaseIngestLiveLinkDevice::FImpl::IngestTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions, TSharedPtr<UE::CaptureManager::FTaskProgress> InTaskProgress)
{
	if (!TaskWaiter.CreateTask())
	{
		return;
	}

	ON_SCOPE_EXIT
	{
		TaskWaiter.FinishTask();
	};

	TStrongObjectPtr<const UIngestCapability_Options> IngestOptions(InIngestOptions);

	using namespace UE::CaptureManager;
	using namespace UE::CaptureManager::Private;

	int32 TakeId = InProcessHandle->GetTakeId();
	FString TakeFullPath = Owner->GetFullTakePath(TakeId);

	if (TakeFullPath.IsEmpty())
	{
		FString Message = FString::Printf(TEXT("Failed to lookup take directory for take id %d"), TakeId);
		ensureMsgf(false, TEXT("%s"), *Message);
		Owner->ExecuteProcessFinishedReporter(InProcessHandle, MakeError(FIngestCapability_Error::InvalidArgument, MoveTemp(Message)));
		return;
	}

	TOptional<FTakeMetadata> MaybeTakeMetadata = Owner->GetTakeMetadata(TakeId);
	if (!MaybeTakeMetadata.IsSet())
	{
		FString Message = FString::Printf(TEXT("Failed to lookup take metadata for take id %d (TakePath=%s)"), TakeId, *TakeFullPath);
		ensureMsgf(false, TEXT("%s"), *Message);
		Owner->ExecuteProcessFinishedReporter(InProcessHandle, MakeError(FIngestCapability_Error::InvalidArgument, MoveTemp(Message)));
		return;
	}

	FTakeMetadata TakeMetadata = MoveTemp(MaybeTakeMetadata.GetValue());

	FString TakeName = TakeMetadata.Slate + TEXT("_") + FString::FromInt(TakeMetadata.TakeNumber);

	const FString TakeConversionDirectory = IngestOptions->WorkingDirectory;

	FGuid TakeUploadId = FGuid::NewGuid();

	FCaptureDataConverterParams Params;
	Params.TakeName = TakeName;
	Params.TakeMetadata = TakeMetadata;
	Params.TakeOriginDirectory = MoveTemp(TakeFullPath);
	Params.TakeOutputDirectory = FPaths::Combine(TakeConversionDirectory, TakeName + TEXT("_") + TakeUploadId.ToString());

	FCaptureDataConverterResult<void> Result = RunConversion(TakeId, IngestOptions, InTaskProgress, Params);

	{
		FScopeLock ConvertersRemoveLock(&Mutex);
		Converters.Remove(TakeId);
	}

	if (Result.HasValue())
	{
		if (IngestOptions->UploadHostName.IsEmpty())
		{
			TryCleanUpWorkingDirectory(Params.TakeOutputDirectory);

			FString Message = TEXT("The upload endpoint was not specified (it was an empty string), try setting a default in the hub settings");
			Owner->ExecuteProcessFinishedReporter(InProcessHandle, MakeError(FIngestCapability_Error::UnrealEndpointNotFound, MoveTemp(Message)));
			return;
		}

		TSharedRef<FUnrealEndpointManager> UnrealEndpointManager = FModuleManager::LoadModuleChecked<FCaptureManagerUnrealEndpointModule>("CaptureManagerUnrealEndpoint").GetEndpointManager();

		TSharedPtr<FUnrealEndpoint> UnrealEndpoint = UnrealEndpointManager->FindEndpointByPredicate(
			[&IngestOptions](const FUnrealEndpoint& InEndpoint)
			{
				return InEndpoint.GetInfo().HostName == IngestOptions->UploadHostName;
			}
		);

		// We can't upload without an unreal endpoint, so there's no point in proceeding any further
		if (!UnrealEndpoint)
		{
			TryCleanUpWorkingDirectory(Params.TakeOutputDirectory);

			FString Message = FString::Printf(TEXT("Upload failed because the requested endpoint was not found: %s"), *IngestOptions->UploadHostName);
			Owner->ExecuteProcessFinishedReporter(InProcessHandle, MakeError(FIngestCapability_Error::UnrealEndpointNotFound, MoveTemp(Message)));
			return;
		}

		// We wait until after the download and conversion to start the connection, in this way we won't require a
		// connection for download only workflows. Note: Disconnection occurs for all endpoints at the end of job
		// processing (in the ingest job processor).
		UnrealEndpoint->StartConnection();

		// Make sure we're not blocking the game thread. This wait should only be noticeable for the first task to
		// connect to a particular endpoint, after that this wait should be negligible for that endpoint.
		check(!IsInGameThread());
		const bool bIsConnected = UnrealEndpoint->WaitForConnectionState(FUnrealEndpoint::EConnectionState::Connected, 3'000);

		if (!bIsConnected)
		{
			TryCleanUpWorkingDirectory(Params.TakeOutputDirectory);

			FString EndpointInfoString = UnrealEndpointInfoToString(UnrealEndpoint->GetInfo());
			FString Message = FString::Printf(TEXT("Upload failed because we timed out connecting to the endpoint: %s"), *EndpointInfoString);
			Owner->ExecuteProcessFinishedReporter(InProcessHandle, MakeError(FIngestCapability_Error::UnrealEndpointConnectionTimedOut, MoveTemp(Message)));
			return;
		}

		TUniquePtr<FTakeUploadTask> TakeUploadTask = MakeUnique<FTakeUploadTask>(
			TakeUploadId,
			Owner->GetDeviceId(),
			Owner->GetDisplayName().ToString(),
			Params.TakeOutputDirectory,
			TakeMetadata
		);

		TakeUploadTask->Progressed().BindLambda(
			// We need to keep a link back to the task progress object, as the upload task refers to it internally
			[TaskProgress = InTaskProgress, UploadTask = InTaskProgress->StartTask()](const double InProgressed) mutable
			{
				UploadTask.Update(InProgressed);
			}
		);

		TakeUploadTask->Complete().BindLambda(
			// We need to keep the Impl alive until the task is complete, as we need to use it
			[Impl = SharedThis(this), ProcessHandle = InProcessHandle, TakeOutputDirectory = Params.TakeOutputDirectory](const FString& InMessage, const int32 InStatusCode)
			{
				TryCleanUpWorkingDirectory(TakeOutputDirectory);

				FScopeLock TakeUploadsRemoveLock(&Impl->Mutex);
				Impl->TakeUploads.Remove(ProcessHandle->GetTakeId());
				TakeUploadsRemoveLock.Unlock();

				if (InStatusCode != 0 || !InMessage.IsEmpty())
				{
					TValueOrError<void, FIngestCapability_Error> Err = MakeError(FIngestCapability_Error::UnrealEndpointUploadError, InMessage);
					Impl->Owner->ExecuteProcessFinishedReporter(ProcessHandle, MoveTemp(Err));
					return;
				}

				Impl->Owner->ExecuteProcessFinishedReporter(ProcessHandle, MakeValue<>());
			}
		);

		FGuid TaskID;
		const bool bAddOk = UnrealEndpoint->AddTakeUploadTask(MoveTemp(TakeUploadTask), TaskID);

		if (bAddOk)
		{
			// Add an entry to the lookup table used for aborting queued upload tasks
			FTakeUploadInfo TakeUploadInfo;
			TakeUploadInfo.TaskId = MoveTemp(TaskID);
			TakeUploadInfo.EndpointID = UnrealEndpoint->GetInfo().EndpointID;

			FScopeLock TakeUploadsEmplaceLock(&Mutex);
			TakeUploads.Emplace(InProcessHandle->GetTakeId(), MoveTemp(TakeUploadInfo));
		}
		else
		{
			ensureMsgf(false, TEXT("Failed to lookup take directory for take id %d"), TakeId);
		}
	}
	else
	{
		TryCleanUpWorkingDirectory(Params.TakeOutputDirectory);

		FCaptureDataConverterError Error = Result.StealError();

		FText Message = FText::Join(FText::FromString(TEXT("\n\r")), Error.GetErrors());
		Owner->ExecuteProcessFinishedReporter(InProcessHandle, MakeError(FIngestCapability_Error::ConversionError, Message.ToString()));
	}
}

void UBaseIngestLiveLinkDevice::FImpl::CancelIngest(int32 InTakeId)
{
	using namespace UE::CaptureManager;

	FScopeLock ScopeLock(&Mutex);

	if (TSharedRef<FCaptureDataConverter>* Converter = Converters.Find(InTakeId))
	{
		(*Converter)->Cancel();
	}

	const FTakeUploadInfo* TakeUploadInfo = TakeUploads.Find(InTakeId);

	if (TakeUploadInfo)
	{
		// Find the endpoint responsible for uploading this task
		TSharedRef<FUnrealEndpointManager> UnrealEndpointManager = FModuleManager::LoadModuleChecked<FCaptureManagerUnrealEndpointModule>("CaptureManagerUnrealEndpoint").GetEndpointManager();

		TSharedPtr<FUnrealEndpoint> Endpoint = UnrealEndpointManager->FindEndpointByPredicate(
			[TakeUploadInfo](const FUnrealEndpoint& InEndpoint) -> bool
			{
				return InEndpoint.GetInfo().EndpointID == TakeUploadInfo->EndpointID;
			}
		);

		if (Endpoint)
		{
			Endpoint->CancelTakeUploadTask(TakeUploadInfo->TaskId);
		}
	}
}


FCaptureDataConverterResult<void> UBaseIngestLiveLinkDevice::FImpl::RunConversion(int32 InTakeId,
																				  TStrongObjectPtr<const UIngestCapability_Options> InIngestOptions,
																				  TSharedPtr<UE::CaptureManager::FTaskProgress> InTaskProgress,
																				  FCaptureDataConverterParams& InParams)
{
	using namespace UE::CaptureManager;

	FTaskProgress::FTask ConvertTask = InTaskProgress->StartTask();

	FCaptureConvertVideoOutputParams VideoParams;
	VideoParams.Format = InIngestOptions->Video.Format;
	VideoParams.ImageFileName = InIngestOptions->Video.FileNamePrefix;
	VideoParams.OutputPixelFormat = Private::ConvertPixelFormat(InIngestOptions->Video.PixelFormat);
	VideoParams.Rotation = Private::ConvertImageRotation(InIngestOptions->Video.Rotation);

	InParams.VideoOutputParams = MoveTemp(VideoParams);

	FCaptureConvertAudioOutputParams AudioParams;
	AudioParams.Format = InIngestOptions->Audio.Format;
	AudioParams.AudioFileName = InIngestOptions->Audio.FileNamePrefix;

	InParams.AudioOutputParams = MoveTemp(AudioParams);

	FCaptureConvertDepthOutputParams DepthParams;
	DepthParams.ImageFileName = TEXT("depth");
	DepthParams.Rotation = Private::ConvertImageRotation(InIngestOptions->Video.Rotation); // Apply the same rotation as video to depth stream

	InParams.DepthOutputParams = MoveTemp(DepthParams);

	FCaptureConvertCalibrationOutputParams CalibrationParams;
	CalibrationParams.FileName = TEXT("calibration");

	InParams.CalibrationOutputParams = MoveTemp(CalibrationParams);

	FCaptureDataConverter::FProgressReporter ProgressReporter =
		FCaptureDataConverter::FProgressReporter::CreateLambda([ConvertTask](const double InProgress) mutable
	{
		ConvertTask.Update(InProgress);
	});

	FScopeLock ScopeLock(&Mutex);
	TSharedRef<FCaptureDataConverter> Converter = Converters.FindOrAdd(InTakeId, MakeShared<FCaptureDataConverter>());
	ScopeLock.Unlock();

	// Blocking function so do not hold the lock for this
	return Converter->Run(InParams, MoveTemp(ProgressReporter));
}

void UBaseIngestLiveLinkDevice::OnDeviceAdded()
{
	Impl = MakeShared<FImpl>(this);
}

void UBaseIngestLiveLinkDevice::OnDeviceRemoved()
{
	Impl->OnDeviceRemoved(Execute_GetTakeIdentifiers(this));

	RemoveAllTakes();

	Super::OnDeviceRemoved();
}

void UBaseIngestLiveLinkDevice::IngestTake(const UIngestCapability_ProcessHandle* InProcessHandle, 
										   const UIngestCapability_Options* InIngestOptions, 
										   TSharedPtr<UE::CaptureManager::FTaskProgress> InTaskProgress)
{
	Impl->IngestTake(InProcessHandle, InIngestOptions, MoveTemp(InTaskProgress));
}

void UBaseIngestLiveLinkDevice::RunDownloadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions)
{
	// No download needed so this step is always a success
	ExecuteProcessProgressReporter(InProcessHandle, 1.f);
	ExecuteProcessFinishedReporter(InProcessHandle, MakeValue());
}

void UBaseIngestLiveLinkDevice::CancelIngest(int32 InTakeId)
{
	Impl->CancelIngest(InTakeId);
}

void UBaseIngestLiveLinkDevice::CancelIngestProcess_Implementation(const UIngestCapability_ProcessHandle* InProcessHandle)
{
	Impl->CancelIngest(InProcessHandle->GetTakeId());
}