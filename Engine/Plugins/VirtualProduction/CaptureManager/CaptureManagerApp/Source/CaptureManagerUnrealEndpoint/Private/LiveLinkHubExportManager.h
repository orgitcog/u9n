// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubExportClient.h"

class FLiveLinkHubExportManager
{
public:

	FLiveLinkHubExportManager(FGuid InClientId,
							  FLiveLinkHubExportClient::FOnDataUploaded InDataUploaded,
							  uint64 InDefaultNumberOfWorkers = DefaultNumberOfWorkers);

	~FLiveLinkHubExportManager();

	[[nodiscard]] int32 ExportTake(const FLiveLinkHubExportClient::FTakeUploadParams& InTakeUploadParams,
								   const FString& InTakeDirectory,
								   const FTakeMetadata& InTakeMetadata);

	void AbortExport(int32 InWorkerId);

	FGuid GetClientId() const;

private:

	static const uint64 DefaultNumberOfWorkers = 2;

	int32 GetPreferredWorkerIndex() const;

	FGuid ClientId;

	FCriticalSection Mutex;
	TArray<TSharedPtr<FLiveLinkHubExportClient>> Workers;
};
