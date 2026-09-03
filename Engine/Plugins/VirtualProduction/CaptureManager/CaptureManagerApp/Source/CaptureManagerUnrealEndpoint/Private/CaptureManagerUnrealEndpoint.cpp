// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerUnrealEndpoint.h"
#include "CaptureManagerUnrealEndpointLog.h"

#include "CaptureCommunicationManager.h"

#include <condition_variable>

namespace UE::CaptureManager
{

struct FUnrealEndpoint::FImpl : public TSharedFromThis<FUnrealEndpoint::FImpl>
{
	FImpl(FUnrealEndpointInfo EndpointInfo);
	~FImpl();

	void SetupConnectionDelegate();

	void StartConnection();
	void StopConnection();

	bool WaitForConnectionState(const FUnrealEndpoint::EConnectionState InConnectionState, int32 InTimeoutMs) const;

	bool AddTakeUploadTask(TUniquePtr<FTakeUploadTask> InTakeUploadTask, FGuid& OutTaskID);
	void CancelTakeUploadTask(FGuid InTakeUploadTask);
	const FUnrealEndpointInfo& GetInfo() const;

	FCriticalSection CriticalSection;
	mutable std::condition_variable CondVar;
	mutable std::mutex CondMtx;

	const FUnrealEndpointInfo EndpointInfo;
	std::atomic<bool> bConnectionIsStarted;
	bool bIsConnected;

	struct FUploadTrackingInfo
	{
		int32 UploadID = -1;
		TWeakPtr<FUploader> Uploader;
	};

	TArray<TSharedRef<FTakeUploadTask>> TakeUploadTasks;
	TMap<FGuid, FUploadTrackingInfo> UploadTrackingInfoByTaskID;
	FCommunicationManager CommunicationManager;
};

FUnrealEndpoint::FUnrealEndpoint(FUnrealEndpointInfo EndpointInfo) :
	Impl(MakeShared<FImpl>(EndpointInfo))
{
	Impl->SetupConnectionDelegate();
}

FUnrealEndpoint::~FUnrealEndpoint() = default;

void FUnrealEndpoint::StartConnection()
{
	Impl->StartConnection();
}

void FUnrealEndpoint::StopConnection()
{
	Impl->StopConnection();
}

bool FUnrealEndpoint::WaitForConnectionState(const FUnrealEndpoint::EConnectionState InConnectionState, int32 InTimeoutMs) const
{
	return Impl->WaitForConnectionState(InConnectionState, InTimeoutMs);
}

bool FUnrealEndpoint::AddTakeUploadTask(TUniquePtr<FTakeUploadTask> InTakeUploadTask, FGuid& OutTaskID)
{
	return Impl->AddTakeUploadTask(MoveTemp(InTakeUploadTask), OutTaskID);
}

void FUnrealEndpoint::CancelTakeUploadTask(FGuid InTakeUploadTask)
{
	Impl->CancelTakeUploadTask(MoveTemp(InTakeUploadTask));
}

const FUnrealEndpointInfo& FUnrealEndpoint::GetInfo() const
{
	return Impl->GetInfo();
}

FUnrealEndpoint::FImpl::FImpl(FUnrealEndpointInfo InEndpointInfo) :
	EndpointInfo(MoveTemp(InEndpointInfo)),
	bConnectionIsStarted(false),
	bIsConnected(false)
{
}

FUnrealEndpoint::FImpl::~FImpl() = default;

void FUnrealEndpoint::FImpl::SetupConnectionDelegate()
{
	CommunicationManager.ConnectionStateChanged().BindLambda(
		[This = AsShared()](const FMessageAddress& InMessageAddress, bool bInIsConnected)
		{
			if (InMessageAddress == This->EndpointInfo.MessageAddress)
			{
				{
					std::lock_guard<std::mutex> LockGuard(This->CondMtx);
					This->bIsConnected = bInIsConnected;
				}
				This->CondVar.notify_one();
			}
		}
	);
}

void FUnrealEndpoint::FImpl::StartConnection()
{
	if (bConnectionIsStarted)
	{
		return;
	}

	bConnectionIsStarted = true;

	FScopeLock ScopeLock(&CriticalSection);
	CommunicationManager.Connect(EndpointInfo.MessageAddress, EndpointInfo.IPAddress, EndpointInfo.ImportServicePort);
}

void FUnrealEndpoint::FImpl::StopConnection()
{
	if (!bConnectionIsStarted)
	{
		// Nothing to do
		return;
	}

	FScopeLock ScopeLock(&CriticalSection);
	CommunicationManager.Disconnect();
	bConnectionIsStarted = false;
}

bool FUnrealEndpoint::FImpl::WaitForConnectionState(const FUnrealEndpoint::EConnectionState InConnectionState, const int32 TimeoutMs) const
{
	std::unique_lock<std::mutex> Lock(CondMtx);

	bool bWaitSuccess = CondVar.wait_for(
		Lock,
		std::chrono::milliseconds(TimeoutMs),
		[This = AsShared(), InConnectionState]() -> bool
		{
			switch (InConnectionState)
			{
				case EConnectionState::Connected:
					return This->bIsConnected;

				case EConnectionState::Disconnected:
					return !This->bIsConnected;

				default:
					check(false);
					return false;
			}
		}
	);

	return bWaitSuccess;
}

bool FUnrealEndpoint::FImpl::AddTakeUploadTask(TUniquePtr<FTakeUploadTask> InTakeUploadTask, FGuid& OutTaskID)
{
	if (!InTakeUploadTask)
	{
		check(false);
		return false;
	}

	// Convert the task into a shared ref, so we can share it in callbacks
	TSharedRef<FTakeUploadTask> SharedTask = MakeShareable<FTakeUploadTask>(InTakeUploadTask.Release());

	// Note: Only the first call to GetUploader will register these callbacks
	FUploader::FUploadStateUpdate UpdateCallback = FUploader::FUploadStateUpdate::CreateLambda(
		[This = AsShared()](const FGuid& InTakeUploadID, const double InProgress)
		{
			// Find the take upload task and update it
			FScopeLock ScopeLock(&This->CriticalSection);

			const int32 TaskIndex = This->TakeUploadTasks.IndexOfByPredicate(
				[&InTakeUploadID](const TSharedRef<FTakeUploadTask>& InTask)
				{
					return InTask->GetTaskID() == InTakeUploadID;
				}
			);

			if (TaskIndex != INDEX_NONE)
			{
				// Grab a strong reference before releasing the lock
				TSharedRef<FTakeUploadTask> ThisTask = This->TakeUploadTasks[TaskIndex];
				ScopeLock.Unlock();

				ThisTask->Progressed().ExecuteIfBound(InProgress);
			}
		}
	);

	// Note: Only the first call to GetUploader will register these callbacks
	FUploader::FUploadFinished UpdateFinishedCallback = FUploader::FUploadFinished::CreateLambda(
		[This = AsShared()](const FGuid& InTakeUploadID, const FString& InMessage, int32 InCode)
		{
			FScopeLock ScopeLock(&This->CriticalSection);

			// Find the take upload task and update it
			const int32 TaskIndex = This->TakeUploadTasks.IndexOfByPredicate(
				[&InTakeUploadID](const TSharedRef<FTakeUploadTask>& InTask)
				{
					return InTask->GetTaskID() == InTakeUploadID;
				}
			);

			if (TaskIndex != INDEX_NONE)
			{
				// Grab a strong reference before releasing the lock
				TSharedRef<FTakeUploadTask> ThisTask = This->TakeUploadTasks[TaskIndex];
				This->TakeUploadTasks.RemoveAt(TaskIndex);
				ScopeLock.Unlock();

				ThisTask->Complete().ExecuteIfBound(InMessage, InCode);
			}
		}
	);

	FScopeLock ScopeLock(&CriticalSection);

	TWeakPtr<FUploader> WeakUploader = CommunicationManager.GetUploader(
		SharedTask->GetCaptureSourceID(),
		SharedTask->GetCaptureSourceName(),
		MoveTemp(UpdateCallback),
		MoveTemp(UpdateFinishedCallback)
	);

	TSharedPtr<FUploader> Uploader = WeakUploader.Pin();

	if (!Uploader)
	{
		const FString TakeInfoString = FString::Printf(
			TEXT("Slate=%s, Take=%d, UniqueID=%s)"),
			*SharedTask->GetTakeMetadata().Slate,
			SharedTask->GetTakeMetadata().TakeNumber,
			*SharedTask->GetTakeMetadata().UniqueId
		);
		UE_LOG(LogCaptureManagerUnrealEndpoint, Error, TEXT("Failed to find an uploader for %s"), *TakeInfoString);

		return false;
	}

	const FGuid& TaskID = SharedTask->GetTaskID();
	const int32 UploadID = Uploader->UploadTake(TaskID, SharedTask->GetDataDirectory(), SharedTask->GetTakeMetadata());

	FUploadTrackingInfo UploadTrackingInfo;
	UploadTrackingInfo.UploadID = UploadID;
	UploadTrackingInfo.Uploader = WeakUploader;
	UploadTrackingInfoByTaskID.Emplace(TaskID, MoveTemp(UploadTrackingInfo));

	TakeUploadTasks.Emplace(MoveTemp(SharedTask));

	OutTaskID = TaskID;
	return true;
}

void FUnrealEndpoint::FImpl::CancelTakeUploadTask(const FGuid InTakeUploadTaskId)
{
	FScopeLock ScopeLock(&CriticalSection);

	const FUploadTrackingInfo* UploadTrackingInfo = UploadTrackingInfoByTaskID.Find(InTakeUploadTaskId);

	if (UploadTrackingInfo)
	{
		TSharedPtr<FUploader> SharedUploader = UploadTrackingInfo->Uploader.Pin();

		if (SharedUploader)
		{
			SharedUploader->AbortUpload(UploadTrackingInfo->UploadID);
		}
	}
}

const FUnrealEndpointInfo& FUnrealEndpoint::FImpl::GetInfo() const
{
	return EndpointInfo;
}

FTakeUploadTask::FUploadProgressed& FTakeUploadTask::Progressed()
{
	return ProgressedDelegate;
}

FTakeUploadTask::FUploadComplete& FTakeUploadTask::Complete()
{
	return CompleteDelegate;
}

FTakeUploadTask::FTakeUploadTask(FGuid InTaskID, FGuid InCaptureSourceID, FString InCaptureSourceName, FString InDataDirectory, FTakeMetadata InTakeMetadata) :
	TaskID(MoveTemp(InTaskID)),
	CaptureSourceID(MoveTemp(InCaptureSourceID)),
	CaptureSourceName(MoveTemp(InCaptureSourceName)),
	DataDirectory(MoveTemp(InDataDirectory)),
	TakeMetadata(MoveTemp(InTakeMetadata))
{
}

const FGuid& FTakeUploadTask::GetTaskID() const
{
	return TaskID;
}

const FGuid& FTakeUploadTask::GetCaptureSourceID() const
{
	return CaptureSourceID;
}

const FString& FTakeUploadTask::GetCaptureSourceName() const
{
	return CaptureSourceName;
}

const FString& FTakeUploadTask::GetDataDirectory() const
{
	return DataDirectory;
}

const FTakeMetadata& FTakeUploadTask::GetTakeMetadata() const
{
	return TakeMetadata;
}

FString UnrealEndpointInfoToString(const FUnrealEndpointInfo& InEndpointInfo)
{
	return FString::Printf(
		TEXT("%s:%d (%s) - %s"),
		*InEndpointInfo.IPAddress,
		InEndpointInfo.ImportServicePort,
		*InEndpointInfo.HostName,
		*InEndpointInfo.EndpointID.ToString(EGuidFormats::DigitsWithHyphens)
	);
}

}
