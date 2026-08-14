// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IngestJob.h"

class UIngestCapability_ProcessHandle;

namespace UE::CaptureManager
{
class FIngestJobQueue;

class FIngestJobExecutor : public FRunnable
{
public:
	DECLARE_DELEGATE(FOnComplete);
	DECLARE_DELEGATE_TwoParams(FJobProcessingStateChanged, FGuid, FIngestJob::EProcessingState);

	FIngestJobExecutor(FString InExecutorName, TSharedRef<FIngestJobQueue> InProcessingQueue, FOnComplete InOnComplete, FJobProcessingStateChanged InJobProcessingStateChanged);
	~FIngestJobExecutor();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	void CancelForDevice(const FGuid& InDeviceId);

private:
	struct FRunIngestContext
	{
		TStrongObjectPtr<ULiveLinkDevice> Device;
		TStrongObjectPtr<UIngestCapability_ProcessHandle> ProcessHandle;
	};

	TUniquePtr<FIngestJobExecutor::FRunIngestContext> RunIngest(FIngestJob& InJob, TPromise<void>& InPromise);

	FCriticalSection CriticalSection;
	FString ExecutorName;
	TSharedRef<FIngestJobQueue> ProcessingQueue;
	TUniquePtr<FIngestJobExecutor::FRunIngestContext> RunIngestProcessContext;
	FOnComplete Complete;
	FJobProcessingStateChanged JobProcessingStateChanged;

	std::atomic<bool> bIsRunning;
	std::atomic<bool> bStopRequested;
	TUniquePtr<FRunnableThread> Thread;
};

}
