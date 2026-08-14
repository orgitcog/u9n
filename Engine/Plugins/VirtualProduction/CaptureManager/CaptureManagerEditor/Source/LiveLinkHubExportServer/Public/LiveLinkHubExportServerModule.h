// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "LiveLinkHubExportServer.h"

class LIVELINKHUBEXPORTSERVER_API FLiveLinkHubExportServerModule
	: public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool StartExportServer(uint16 InPort);
	bool StopExportServer();
	bool IsExportServerRunning() const;

	TValueOrError<FLiveLinkHubExportServer::FServerInfo, FLiveLinkHubExportServer::EServerError> GetExportServerInfo() const;

	void RegisterExportServerHandler(FString InClientId, FLiveLinkHubExportServer::FFileDataHandler InFileDataHandler);
	void UnregisterExportServerHandler(FString InClientId);

private:

	TSharedPtr<FLiveLinkHubExportServer> ExportServer;
};