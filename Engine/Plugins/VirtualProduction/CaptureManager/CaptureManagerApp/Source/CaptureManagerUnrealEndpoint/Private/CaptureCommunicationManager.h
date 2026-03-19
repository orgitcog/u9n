// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messenger.h"
#include "Features/ConnectStarter.h"
#include "Features/UploadStateHandler.h"

#include "LiveLinkHubExportManager.h"

#include "CaptureManagerTakeMetadata.h"

class FUploader
{
private:

	struct FPrivateToken
	{
		explicit FPrivateToken() = default;
	};

public:

	DECLARE_DELEGATE_TwoParams(FUploadStateUpdate, const FGuid& InTakeUploadId, double InProgress);
	DECLARE_DELEGATE_ThreeParams(FUploadFinished, const FGuid& InTakeUploadId, const FString& InMessage, int32 InCode);

	FUploader(FPrivateToken, FGuid InClientGuid, FGuid InCaptureSourceId, FString InCaptureSourceName, FString InIpAddress, uint16 InPort);

	int32 UploadTake(const FGuid& InTakeUploadId,
					 const FString& InTakeDirectory,
					 const FTakeMetadata& InTakeMetadata);

	void AbortUpload(int32 InUploadId);

private:

	void SetUploadHandlers(FUploadStateUpdate InUploadUpdate,
						   FUploadFinished InUploadFinished);

	void OnDataUploadFinished(const FGuid& InTakeUploadId, FUploadVoidResult InResult);

	FGuid CaptureSourceId;
	FString CaptureSourceName;

	FString IpAddress;
	uint16 Port;

	FUploadStateUpdate UploadUpdate;
	FUploadFinished UploadFinished;

	TUniquePtr<FLiveLinkHubExportManager> ExportManager;

	friend class FCommunicationManager;
};

class FCommunicationManager
{
public:

	DECLARE_DELEGATE_TwoParams(FConnectionStateChanged, FMessageAddress InClientAddress, bool bIsConnected);

	FCommunicationManager();
	~FCommunicationManager();

	void Connect(FMessageAddress InAddress, FString InIpAddress, uint16 InPort);
	void Disconnect();

	TWeakPtr<FUploader> GetUploader(const FGuid& InCaptureSourceId,
									const FString& InCaptureSourceName,
									FUploader::FUploadStateUpdate InUploadUpdate,
									FUploader::FUploadFinished InUploadFinished);
	void RemoveUploader(FGuid InCaptureSourceId);

	bool IsConnected() const;

	FConnectionStateChanged& ConnectionStateChanged();

private:

	using FCommMessenger = FMessenger<FConnectStarter, FUploadStateHandler>;

	void OnDisconnect();
	void OnUploadState(const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, double InProgress);
	void OnUploadFinished(const FGuid& InCaptureSourceId, const FGuid& InTakeUploadId, FString InMessage, int32 InCode);

	TWeakPtr<FUploader> FindUploader(const FGuid& InCaptureSourceId);
	TWeakPtr<FUploader> AddUploader(const FGuid& InCaptureSourceId,
									const FString& InCaptureSourceName,
									FUploader::FUploadStateUpdate InUploadUpdate,
									FUploader::FUploadFinished InUploadFinished);

	FConnectionStateChanged ConnectionStateChangedDelegate;

	TSharedPtr<FCommMessenger> Messenger;
	
	FCriticalSection Mutex;
	TMap<FGuid, TSharedPtr<FUploader>> Uploaders;

	FString IpAddress;
	uint16 Port = 0;
};
