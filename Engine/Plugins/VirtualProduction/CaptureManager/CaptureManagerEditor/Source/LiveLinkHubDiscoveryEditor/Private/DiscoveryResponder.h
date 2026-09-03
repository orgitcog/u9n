// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

#include "LiveLinkHubCaptureMessages.h"
#include "LiveLinkHubExportServer.h"
#include "LiveLinkHubWorkerManager.h"

#include "MessageEndpoint.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLiveLinkHubDiscoveryEditor, Log, All)

namespace UE::CaptureManager
{

class FDiscoveryResponder
{
public:
	FDiscoveryResponder();
	~FDiscoveryResponder();

private:

	static TValueOrError<FLiveLinkHubExportServer::FServerInfo, FLiveLinkHubExportServer::EServerError> GetExportServerInfo();
	static TSharedRef<FLiveLinkHubWorkerManager> GetWorkerManager();

	void HandleDiscoveryRequest(
		const FDiscoveryRequest& InRequest,
		const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext
	);

	void StartDiscoveryResponder();

	TSharedRef<FLiveLinkHubWorkerManager> Manager;

	FString HostName;
	TOptional<FLiveLinkHubExportServer::FServerInfo> CachedServerInfo;

	TSharedPtr<FMessageEndpoint> MessageEndpoint;

	std::atomic_bool bStarted = false;
};

} // namespace UE::CaptureManager