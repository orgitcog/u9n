// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExportClient/Communication/ExportCommunication.h"
#include "ExportWorker.h"

#include "Containers/Map.h"

namespace UE::CaptureManager
{

class FExportTaskStopToken
{
public:

	FExportTaskStopToken();

	void Cancel();
	bool IsCanceled();

private:

	std::atomic_bool bCanceled;
};

class CAPTUREPROTOCOLSTACK_API FExportClient final
	: public FExportTaskExecutor
{
public:

	using FTaskId = uint32;

	using FTakeFileArray = TArray<FTakeFile>;

	FExportClient(FString InServerIp, const uint16 InExportPort);
	~FExportClient();

	FTaskId ExportTakeFiles(FString InTakeName, FTakeFileArray InTakeFileArray, TUniquePtr<FBaseStream> InStream);
	FTaskId ExportFiles(TMap<FString, FTakeFileArray> InTakesFilesMap, TUniquePtr<FBaseStream> InStream);

	void AbortExport(const FTaskId InTaskId);
	void AbortAllExports();

private:

	FTaskId StartExport(FExportTakeTask::FExportContexts InContexts, TUniquePtr<FBaseStream> InStream);

	virtual void OnTask(TUniquePtr<FExportTakeTask> InTask) override;
	bool OnExportTask(TUniquePtr<FExportTakeTask> InTask);

	TProtocolResult<TMap<uint32, TPair<FString, FTakeFile>>> SendRequests(FExportTakeTask::FExportContexts InContexts);
	TProtocolResult<void> ReceiveResponses(TMap<uint32, TPair<FString, FTakeFile>> InResponseMap,
										   FBaseStream& InBaseStream);

	TProtocolResult<void> Start();
	TProtocolResult<void> Stop();

	FString ServerIp;
	uint16 ServerPort = 0;
	TUniquePtr<FExportCommunication> Communication;

	FExportWorker ExportWorker;
	TUniquePtr<FRunnableThread> Thread;

	std::atomic<FTaskId> CurrentTaskId;

	uint32 TransactionIdCounter = 0;

	TUniquePtr<FExportTaskStopToken> CurrentTaskStopToken;
};

}