// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FootageIngest.h"

#include "MetaHumanCaptureError.h"
#include "Error/Result.h"
#include "Async/Task.h"
#include "Utils/IngestAssetCreator.h"
#include "Async/StopToken.h"

class FFileFootageIngest
	: public FFootageIngest
{
public:

	FFileFootageIngest(const FString& InInputDirectory);
	virtual ~FFileFootageIngest();

	//~ FFootageIngest interface
	virtual void Startup(ETakeIngestMode InMode);
	virtual void Shutdown() override;
	virtual int32 GetNumTakes() const override;
	virtual TArray<TakeId> GetTakeIds() const override;
	virtual FMetaHumanTakeInfo GetTakeInfo(TakeId InId) const override;
	virtual void GetTakes(const TArray<TakeId>& InIdList, TPerTakeCallback<void> InCallback) override;
	virtual void RefreshTakeListAsync(TCallback<void> InCallback) override;
	virtual void CancelProcessing(const TArray<TakeId>& InIdList);

protected:

	FString InputDirectory;

	std::atomic_int32_t CurrId = 0;
	mutable FCriticalSection TakeInfoCacheMutex;
	TMap<TakeId, FMetaHumanTakeInfo> TakeInfoCache;
	TMap<TakeId, FStopToken> TakeIngestStopTokens;

	TUniquePtr<FAbortableAsyncTask> RefreshTakeListTask;

	// Functions to be implemented by inheriting class 
	virtual FMetaHumanTakeInfo ReadTake(const FString& InFilePath, const FStopToken& InStopToken, TakeId InNewTakeId) = 0;
	virtual TResult<void, FMetaHumanCaptureError> CreateAssets(const FMetaHumanTakeInfo& InTakeInfo, const FStopToken& InStopToken, FCreateAssetsData& OutCreateAssetsData) = 0;

	void AddTakeInfo(FMetaHumanTakeInfo&& InTakeInfo);
	FMetaHumanTakeInfo GetCachedTakeInfo(TakeId InId) const;
	int32 ClearTakeInfoCache();

	UE::MetaHuman::Pipeline::FPipeline Pipeline;
	int32 TakeIdInPipeline = INVALID_ID;

	void FrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData, TakeId InId);
	void ProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData, TRetainedRef<TResult<void, FMetaHumanCaptureError>> OutResult);

	TResult<void, FMetaHumanCaptureError> RunPipeline(const FStopToken& InStopToken, TakeId InId, bool bInShouldRunMultiThreaded);
	void GetTakesProcessing(const TArray<TakeId>& InTakeIdList, TPerTakeCallback<void> InCallback, const FStopToken& InStopToken);
	
private:

	void DeleteDataForTake(int32 InTakeIndex);
	TakeId GenerateNewTakeId();

	TResult<void, FMetaHumanCaptureError> ReadTakeList(const FStopToken& InStopToken);
};