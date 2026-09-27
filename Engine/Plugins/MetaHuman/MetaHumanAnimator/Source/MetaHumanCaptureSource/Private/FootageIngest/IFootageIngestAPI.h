// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanTakeData.h"

#include "Utils/CommandHandler.h"
#include "Error/Result.h"
#include "Async/EventSourceUtils.h"
#include "MetaHumanCaptureEvents.h"
#include "MetaHumanCaptureError.h"

class IFootageIngestAPI 
	: public FCommandHandler, public FCaptureEventSource
{
public:
	DECLARE_DELEGATE(FOnStartupFinished)
	DECLARE_DELEGATE_OneParam(FOnGetTakesFinished, const TArray<FMetaHumanTake>& InTakes)

	template<typename Type>
	using TCallback = TManagedDelegate<TResult<Type, FMetaHumanCaptureError>>;

	template<typename Type>
	using TPerTakeResult = TPair<TakeId, TResult<Type, FMetaHumanCaptureError>>;

	template<typename Type>
	using TPerTakeCallback = TManagedDelegate<TPerTakeResult<Type>>;

	virtual FOnGetTakesFinished& OnGetTakesFinished() = 0;

	virtual void Startup(ETakeIngestMode InMode = ETakeIngestMode::Async) = 0;
	virtual void SetTargetPath(const FString& InTargetIngestDirectory, const FString& InTargetPackagePath) = 0;
	virtual void Shutdown() = 0;

	virtual bool IsProcessing() const = 0;

	virtual bool IsCancelling() const = 0;
	virtual void CancelProcessing(const TArray<TakeId>& InIdList) = 0;

	/** Returns the progress of the current task */
	virtual float GetTaskProgress(TakeId InId) const = 0;
	virtual FText GetTaskName(TakeId InId) const = 0;

	virtual void RefreshTakeListAsync(TCallback<void> InCallback) = 0;
	virtual int32 GetNumTakes() const = 0;
	virtual TArray<TakeId> GetTakeIds() const = 0;
	virtual FMetaHumanTakeInfo GetTakeInfo(TakeId InId) const = 0;

	virtual void GetTakes(const TArray<TakeId>& InIdList, TPerTakeCallback<void> InCallback) = 0;

	virtual ~IFootageIngestAPI() = default;
};