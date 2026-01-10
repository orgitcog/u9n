// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messenger.h"
#include "Features/ConnectAcceptor.h"
#include "Features/UploadStateSender.h"

#include "MessageEndpoint.h"

#include "LiveLinkHubCaptureMessages.h"

#include "LiveLinkHubExportServer.h"
#include "UploadDataMessage.h"

#include "Async/StopToken.h"

#include "Async/TaskProgress.h"

struct FCaptureDataTakeInfo;

namespace UE::CaptureManager
{
struct FCaptureDataAssetInfo;

namespace Private
{

class FLiveLinkHubImportWorker : public TSharedFromThis<FLiveLinkHubImportWorker>
{
private:

	struct FPrivateToken {};

public:
	using FEditorMessenger = FMessenger<FConnectAcceptor, FUploadStateSender>;

	static TSharedPtr<FLiveLinkHubImportWorker> Create(TWeakPtr<FEditorMessenger> InEditorMessenger);

	explicit FLiveLinkHubImportWorker(TWeakPtr<FEditorMessenger> InEditorMessenger, FPrivateToken);
	~FLiveLinkHubImportWorker();

private:

	struct FTakeFileContext
	{
		uint64 TotalLength = 0;
		uint64 RemainingLength = 0;
		float Progress = 0.f;
	};


	FString EvaluateSettings(const FUploadDataHeader& InHeader);
	bool HandleTakeDownload(FUploadDataHeader InHeader, TSharedPtr<UE::CaptureManager::FTcpClientHandler> InClient);
	FUploadVoidResult HandleFileDownload(const FString& InTakeStoragePath,
		const FUploadFileDataHeader& InFileHeader,
		FTakeFileContext& InContext,
		TSharedPtr<UE::CaptureManager::FTcpClientHandler> InClient,
		UE::CaptureManager::FTaskProgress::FTask& InTask);
	void DeleteDownloadedData(const FString& InTakeStoragePath);

	FTakeFileContext& AddOrIgnoreContext(const FGuid& InUploadId, uint64 InTotalLength);
	void RemoveContext(const FGuid& InUploadId);

	static void SpawnIngestTask(TSharedPtr<FEditorMessenger> InMessenger,
		const FString& InDataStorage,
		const FGuid& InCaptureSourceId,
		const FString& InCaptureSourceName,
		const FGuid& InTakeUploadId,
		TSharedPtr<UE::CaptureManager::FTaskProgress> InTaskProgress);

	static bool CreateCaptureAsset(const FString& InAssetPath,
		const UE::CaptureManager::FCaptureDataAssetInfo& InResult,
		const FCaptureDataTakeInfo& InTakeInfo);

	static void SaveCaptureCreatedAssets(const FString& InAssetPath);

	FCriticalSection ExportMutex;
	TMap<FGuid, FTakeFileContext> TakeFilesContext;

	TWeakPtr<FEditorMessenger> Messenger;
};

}
}