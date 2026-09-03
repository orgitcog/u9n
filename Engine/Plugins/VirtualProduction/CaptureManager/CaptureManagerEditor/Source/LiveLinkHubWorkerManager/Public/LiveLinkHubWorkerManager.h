// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubWorker.h"


class LIVELINKHUBWORKERMANAGER_API FLiveLinkHubWorkerManager
{
public:

	FLiveLinkHubWorkerManager();
	~FLiveLinkHubWorkerManager();

	void Disconnect();
	bool IsConnected() const;
	void SendDiscoveryResponse(FDiscoveryResponse* Response, FMessageAddress Receiver);

private:

	TWeakPtr<UE::CaptureManager::Private::FLiveLinkHubImportWorker> AddWorker(FMessageAddress InServer);

	void RemoveWorker(FMessageAddress InServer);

	FConnectResponse* ConnectAccepted(const FConnectRequest& InRequest, const FMessageAddress& InAddress);
	void ConnectionLost(const FMessageAddress& InAddress);

	TSharedPtr<UE::CaptureManager::Private::FLiveLinkHubImportWorker::FEditorMessenger> Messenger;

	FCriticalSection Mutex;
	TMap<FString, TSharedPtr<UE::CaptureManager::Private::FLiveLinkHubImportWorker>> Workers;
};