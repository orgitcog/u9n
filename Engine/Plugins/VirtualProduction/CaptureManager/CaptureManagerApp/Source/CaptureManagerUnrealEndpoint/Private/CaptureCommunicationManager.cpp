// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureCommunicationManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogUploader, Log, All);

FUploader::FUploader(FPrivateToken, FGuid InClientGuid, FGuid InCaptureSourceId, FString InCaptureSourceName, FString InIpAddress, uint16 InPort)
	: CaptureSourceId(InCaptureSourceId)
	, CaptureSourceName(InCaptureSourceName)
	, IpAddress(MoveTemp(InIpAddress))
	, Port(InPort)
{
	ExportManager =
		MakeUnique<FLiveLinkHubExportManager>(InClientGuid,
											  FLiveLinkHubExportClient::FOnDataUploaded::CreateRaw(this, &FUploader::OnDataUploadFinished));
}

void FUploader::SetUploadHandlers(FUploadStateUpdate InUploadUpdate,
								  FUploadFinished InUploadFinished)
{
	UploadUpdate = MoveTemp(InUploadUpdate);
	UploadFinished = MoveTemp(InUploadFinished);
}

int32 FUploader::UploadTake(const FGuid& InTakeUploadId,
							const FString& InTakeDirectory,
							const FTakeMetadata& InTakeMetadata)
{
	return ExportManager->ExportTake({ CaptureSourceId, CaptureSourceName, InTakeUploadId, IpAddress, Port }, InTakeDirectory, InTakeMetadata);
}

void FUploader::AbortUpload(int32 InUploadId)
{
	ExportManager->AbortExport(InUploadId);
}

void FUploader::OnDataUploadFinished(const FGuid& InTakeUploadId, FUploadVoidResult InResult)
{
	if (InResult.HasError())
	{
		FUploadError UploadError = InResult.StealError();

		UploadFinished.ExecuteIfBound(InTakeUploadId, UploadError.GetText().ToString(), UploadError.GetCode());
	}
}

FCommunicationManager::FCommunicationManager()
{
	Messenger = MakeShared<FCommMessenger>();

	Messenger->SetDisconnectHandler(FConnectStarter::FDisconnectHandler::CreateRaw(this, &FCommunicationManager::OnDisconnect));
	Messenger->SetUploadCallbacks(FUploadStateHandler::FUploadStateCallback::CreateRaw(this, &FCommunicationManager::OnUploadState),
								  FUploadStateHandler::FUploadFinishedCallback::CreateRaw(this, &FCommunicationManager::OnUploadFinished));
}

FCommunicationManager::~FCommunicationManager()
{
	Messenger->SetDisconnectHandler(nullptr);
	Messenger->SetUploadCallbacks(nullptr, nullptr);

	Messenger = nullptr;

	FScopeLock Lock(&Mutex);
	Uploaders.Empty();
}

void FCommunicationManager::Connect(FMessageAddress InAddress, FString InIpAddress, uint16 InPort)
{
	if (Messenger && Messenger->IsConnected())
	{
		return;
	}

	Messenger->SetAddress(MoveTemp(InAddress));
	Messenger->Connect(
		FConnectStarter::FConnectHandler::CreateLambda(
			[this, IpAddressArg = MoveTemp(InIpAddress), PortArg = InPort](const FConnectResponse& InResponse) mutable
	{
		if (InResponse.Status == EStatus::Ok)
		{
			IpAddress = MoveTemp(IpAddressArg);
			Port = PortArg;

			UE_LOG(LogUploader, Display, TEXT("Connected to the client: %s, export IP address: %s:%d"), *Messenger->GetAddress().ToString(), *IpAddress, Port);

			ConnectionStateChangedDelegate.ExecuteIfBound(Messenger->GetAddress(), true);
		}
		else
		{
			UE_LOG(LogUploader, Error, TEXT("Failed to connect to the client: %s"), *Messenger->GetAddress().ToString());
		}
	}));
}

void FCommunicationManager::Disconnect()
{
	if (Messenger && !Messenger->IsConnected())
	{
		return;
	}

	Messenger->SetDisconnectHandler(
		FConnectStarter::FDisconnectHandler::CreateLambda(
		[this]()
		{
			ConnectionStateChangedDelegate.ExecuteIfBound(Messenger->GetAddress(), false);
		}
	)
	);
	Messenger->Disconnect();

	FScopeLock Lock(&Mutex);
	Uploaders.Empty();
}

TWeakPtr<FUploader> FCommunicationManager::GetUploader(const FGuid& InCaptureSourceId,
													   const FString& InCaptureSourceName,
													   FUploader::FUploadStateUpdate InUploadUpdate,
													   FUploader::FUploadFinished InUploadFinished)
{
	if (!Messenger->IsConnected())
	{
		return nullptr;
	}

	FScopeLock Lock(&Mutex);

	if (TWeakPtr<FUploader> Uploader = FindUploader(InCaptureSourceId); Uploader.IsValid())
	{
		return Uploader;
	}

	return AddUploader(InCaptureSourceId, InCaptureSourceName, MoveTemp(InUploadUpdate), MoveTemp(InUploadFinished));
}

void FCommunicationManager::RemoveUploader(FGuid InCaptureSourceId)
{
	FScopeLock Lock(&Mutex);
	TSharedPtr<FUploader> Uploader;
	if (Uploaders.RemoveAndCopyValue(InCaptureSourceId, Uploader))
	{
		Uploader->SetUploadHandlers(nullptr, nullptr);
	}
}

bool FCommunicationManager::IsConnected() const
{
	return Messenger->IsConnected();
}

FCommunicationManager::FConnectionStateChanged& FCommunicationManager::ConnectionStateChanged()
{
	return ConnectionStateChangedDelegate;
}

TWeakPtr<FUploader> FCommunicationManager::FindUploader(const FGuid& InCaptureSourceId)
{
	if (TSharedPtr<FUploader>* Uploader = Uploaders.Find(InCaptureSourceId))
	{
		return *Uploader;
	}

	return nullptr;
}

TWeakPtr<FUploader> FCommunicationManager::AddUploader(const FGuid& InCaptureSourceId,
													   const FString& InCaptureSourceName,
													   FUploader::FUploadStateUpdate InUploadUpdate,
													   FUploader::FUploadFinished InUploadFinished)
{
	FGuid ClientId;
	FGuid::Parse(Messenger->GetOwnAddress().ToString(), ClientId);

	TSharedPtr<FUploader> UploadContext = MakeShared<FUploader>(FUploader::FPrivateToken(), ClientId, InCaptureSourceId, InCaptureSourceName, IpAddress, Port);
	UploadContext->SetUploadHandlers(MoveTemp(InUploadUpdate), MoveTemp(InUploadFinished));

	TSharedPtr<FUploader>& AddedContext = Uploaders.Add(InCaptureSourceId, MoveTemp(UploadContext));

	return AddedContext;
}

void FCommunicationManager::OnDisconnect()
{
	{
		FScopeLock Lock(&Mutex);
		Uploaders.Empty();
	}
	UE_LOG(LogUploader, Display, TEXT("Disconnected from the client: %s, export IP address: %s:%d"), *Messenger->GetAddress().ToString(), *IpAddress, Port);

	ConnectionStateChangedDelegate.ExecuteIfBound(Messenger->GetAddress(), false);
}

void FCommunicationManager::OnUploadState(const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, double InProgress)
{
	FScopeLock Lock(&Mutex);
	
	if (TSharedPtr<FUploader>* Context = Uploaders.Find(InCaptureSourceId))
	{
		check(*Context);

		// Copying the delegate to avoid calling it while holding a mutex
		FUploader::FUploadStateUpdate UploadUpdate = (*Context)->UploadUpdate;
		Lock.Unlock();

		UploadUpdate.ExecuteIfBound(InTakeUploadId, InProgress);
	}
}

void FCommunicationManager::OnUploadFinished(const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, FString InMessage, int32 InCode)
{
	FScopeLock Lock(&Mutex);

	if (TSharedPtr<FUploader>* Context = Uploaders.Find(InCaptureSourceId))
	{
		check(*Context);

		// Copying the delegate to avoid calling it while holding a mutex
		FUploader::FUploadFinished UploadFinished = (*Context)->UploadFinished;
		Lock.Unlock();

		UploadFinished.ExecuteIfBound(InTakeUploadId, InMessage, InCode);
	}
}
