// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Containers/Ticker.h"

#include "TimerManager.h"

namespace UE::CaptureManager
{

class CAPTUREUTILS_API FCaptureTimerManager
{
public:

	using FTimerHandle = FTSTicker::FDelegateHandle;

	FCaptureTimerManager();
	FCaptureTimerManager(const float Resolution);

	~FCaptureTimerManager();

	FTimerHandle AddTimer(FTimerDelegate InDelegate, float InRate, bool InbLoop = false, float InFirstDelay = 0.f);
	void RemoveTimer(FTimerHandle Handle);

private:
	static constexpr float IdealTimeResolution = 0.1f;

	class FTimerManagerRunnable final : public FRunnable
	{
	public:

		FTimerManagerRunnable(FCaptureTimerManager* InOwner, const float InResolution);

		virtual uint32 Run() override;
		virtual void Stop() override;

	private:

		FCaptureTimerManager* Owner;
		std::atomic_bool bShouldRun = true;
		const float IdealTimeResolution;
	};

	FTSTicker Ticker;
	TUniquePtr<FRunnableThread> Thread;
	TUniquePtr<FTimerManagerRunnable> Runnable;
};

}
