// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FootageIngest.h"
#include "LiveLinkFaceMetadata.h"
#include "Utils/LiveLinkFaceTakeDataConverter.h"
#include "Async/Task.h"

#include "Utils/IngestAssetCreator.h"
#include "Async/StopToken.h"

/////////////////////////////////////////////////////
// FLiveLinkFaceIngestBase

class FLiveLinkFaceIngestBase
	: public FFootageIngest
{
public:

	FLiveLinkFaceIngestBase(bool bInShouldCompressDepthFiles);

	//~ IFootageRetrievalAPI interface
	virtual void Shutdown() override;
	virtual int32 GetNumTakes() const override;
	virtual TArray<TakeId> GetTakeIds() const override;
	virtual FMetaHumanTakeInfo GetTakeInfo(TakeId InId) const override;
	virtual void GetTakes(const TArray<TakeId>& InTakeIdList, TPerTakeCallback<void> InCallback) override;
	virtual void CancelProcessing(const TArray<TakeId>& InTakeIdList) override;

	/** Returns the directory from where takes are ingested from. Used to derive the actual path in the Content folder to place the ingested files */
	virtual const FString& GetTakesOriginDirectory() const = 0;

	// Utility struct to hold information on a take conversion task
	struct FTakeConversionTaskInfo
	{
		FLiveLinkFaceTakeInfo TakeInfo;
		std::atomic<float> Progress;
		FLiveLinkFaceTakeDataConverter::FConvertResult Result;
		bool bCanceled = false;
		bool bHasErrors = false;
		FText ErrorText;
	};

	// a virtual method to get an individual take
	virtual void GetTake(TakeId InTaskTakeId, FTakeConversionTaskInfo& OutTaskInfo);

protected:
	
	static bool IsMetaHumanAnimatorTake(const FString& Directory, const FLiveLinkFaceTakeInfo& TakeInfo);

	void GetTakesProcessing(const TArray<TakeId>& InTakeIdList, TPerTakeCallback<void> InCallback, const FStopToken& InStopToken);
	int32 AddTakeInfo(FLiveLinkFaceTakeInfo&& InTakeInfo);
	FLiveLinkFaceTakeInfo GetLiveLinkFaceTakeInfo(TakeId InId) const;
	int32 ClearTakeInfoCache();
	void RemoveTakeFromTakeCache(TakeId InId);
	FString GetTakeIngestRelativePath(const FLiveLinkFaceTakeInfo& InTakeInfo) const;

	TArray<FCreateAssetsData> PrepareTakeAssets_GameThread(const TArray<FLiveLinkFaceTakeDataConverter::FConvertResult>& InConvertResult, const TArray<FLiveLinkFaceTakeInfo>& InTakeInfoList);

	FLiveLinkFaceTakeInfo ReadTake(const FString& CurrentDirectory);

protected:
	std::atomic_int32_t CurrId = 0;

	mutable FCriticalSection TakeInfoCacheMutex;
	TMap<TakeId, FLiveLinkFaceTakeInfo> TakeInfoCache;

	TMap<TakeId, FStopToken> TakeIngestStopTokens;
	bool bShouldCompressDepthFiles;

private:
	/*
	* ParallelFor uses Background Workers to parallelize its work for each take.
	* We then instantiate 3 threads per each take and we wait until all 3 threads are done.
	* This causes "deadlock" because no more threads can be started to start the per take work, 
	* because all take work is waiting for it to end.
	* 
	* This function calculates the batch size based on how many threads are currently available.
	* 
	* Proper solution would be to not wait until the per take job is finish and therefore remove ParallelFor and 
	* use regular for to execute everything. This work requires a redesign of this and all other class that would be
	* affected by this change.
	*/
	int32 CalculateBatchSize(int32 InTakesToProcess);

	void DeleteDataForTake(TakeId InId);
	TakeId GenerateNewTakeId();
};

/////////////////////////////////////////////////////
// FLiveLinkFaceArchiveIngest

class FLiveLinkFaceArchiveIngest final
	: public FLiveLinkFaceIngestBase
{
public:

	FLiveLinkFaceArchiveIngest(const FString& InInputDirectory, bool bInShouldCompressDepthFiles);
	virtual ~FLiveLinkFaceArchiveIngest();

	//~ IFootageRetrievalAPI interface
	virtual void Startup(ETakeIngestMode InMode) override;
	virtual void Shutdown() override;
	virtual void RefreshTakeListAsync(TCallback<void> InCallback) override;

	//~ FLiveLinkFaceIngestBase interface
	const FString& GetTakesOriginDirectory() const override;

private:
	TResult<void, FMetaHumanCaptureError> ReadTakeList(const FStopToken& InStopToken);

	FString InputDirectory;
	TUniquePtr<FAbortableAsyncTask> RefreshTakeListTask;
};