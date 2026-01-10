// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UploadDataMessage.h"

namespace UE::CaptureManager
{
class FTcpServer;
class FTcpClientHandler;
}

class LIVELINKHUBEXPORTSERVER_API FLiveLinkHubExportServer
{
public:

	DECLARE_DELEGATE_RetVal_TwoParams(bool, FFileDataHandler, FUploadDataHeader, TSharedPtr<UE::CaptureManager::FTcpClientHandler>)

	struct FServerInfo
	{
		FString IPAddress;
		uint16 Port = 0;
	};

	enum class EServerError
	{
		NotRunning,
		InvalidPort,
		InvalidIPAddress
	};

	FLiveLinkHubExportServer();
	~FLiveLinkHubExportServer();

	bool Start();
	bool Start(uint16 InPort);
	bool Stop();

	bool IsRunning() const;

	TValueOrError<FServerInfo, EServerError> GetServerInfo() const;

	void RegisterFileDownloadHandler(FString InClientId, FFileDataHandler InFileDataHandler);
	void UnregisterFileDownloadHandler(FString InClientId);

private:

	class FLiveLinkHubClientExportRunner;

	void OnConnectionChanged(TWeakPtr<UE::CaptureManager::FTcpClientHandler> InClient, bool bIsConnected);
	bool HandleFileData(TSharedPtr<UE::CaptureManager::FTcpClientHandler> InClient, FUploadDataHeader Header);

	TSharedPtr<UE::CaptureManager::FTcpServer> TcpServer;

	FCriticalSection Mutex;
	FCriticalSection HandlersMutex;
	TMap<FString, FFileDataHandler> Handlers; // The key is stringified FGuid
	TMap<FString, TUniquePtr<FLiveLinkHubClientExportRunner>> Runners;
	TMap<FString, TUniquePtr<FRunnableThread>> Threads;
};
